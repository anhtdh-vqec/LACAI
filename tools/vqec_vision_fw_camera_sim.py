#!/usr/bin/env python3
"""Board-side mock of the FW camera service for LACAI integration tests.

It reproduces the two FW responsibilities LACAI depends on:

1. Control: owns the system-bus name `com.vnpt.camera.Camera` and implements
   `com.vnpt.camera.Camera1` StartStream/StopStream/GetStatus with `(a{sv})`
   string dictionaries, so LACAI's `dbus_rpc`/`camera_control` can acquire a
   third-stream lease exactly as against real FW.
2. Media: serves the released raw-frame wire on an AF_UNIX SOCK_SEQPACKET socket
   named `<socket_dir>/<channel>_third_<consumer>.sock` (`0_third_ai.sock`). Each
   message is the 104-byte native-endian FrameHeader plus one FD via SCM_RIGHTS;
   the consumer returns an 8-byte ReturnHeader ACK before the frame is released.

Pixels come from the real Qualcomm camera through qtiqmmfsrc. This fixture copies
NV12 into an ACK-gated memfd pool by default. --dma-heap selects a Linux DMA-BUF
heap to exercise registered input; it still copies pixels and is not released FW.

Run on the target (root), then start the LACAI app against:
  raw_source socket  : <socket_dir>/0_third_ai.sock
  camera D-Bus       : com.vnpt.camera.Camera1 on the system bus
"""

import argparse
import array
import ctypes
import mmap
import os
import queue
import signal
import socket
import struct
import sys
import threading
import time

import gi

gi.require_version("Gst", "1.0")
gi.require_version("GstVideo", "1.0")
gi.require_version("Gio", "2.0")
from gi.repository import Gio, GLib, Gst, GstVideo  # noqa: E402

FRAME_HEADER = struct.Struct("<QIIII4I4iQQQQQQ")
RETURN_HEADER = struct.Struct("<Q")
GST_VIDEO_FORMAT_NV12 = 23
DEFAULT_SOCKET_DIR = "/run/camera_ai"
DEFAULT_BUS_NAME = "com.vnpt.camera.Camera"
DEFAULT_OBJECT_PATH = "/com/vnpt/camera/Camera"
DEFAULT_INTERFACE = "com.vnpt.camera.Camera1"
THREAD_STOP_TIMEOUT_S = 2.0
CODE_OK = 0
CODEC_RAW = "RAW"
DMA_HEAP_ALLOC_LAYOUT = struct.Struct("=QIIQ")
DMA_BUF_SYNC_LAYOUT = struct.Struct("=Q")
IOC_WRITE = 1
IOC_READ = 2
IOC_NR_BITS = 8
IOC_TYPE_BITS = 8
IOC_SIZE_BITS = 14
DMA_HEAP_IOCTL_ALLOC = ((IOC_READ | IOC_WRITE) << (IOC_NR_BITS + IOC_TYPE_BITS + IOC_SIZE_BITS)
                        | DMA_HEAP_ALLOC_LAYOUT.size << (IOC_NR_BITS + IOC_TYPE_BITS)
                        | ord("H") << IOC_NR_BITS)
DMA_BUF_IOCTL_SYNC = (IOC_WRITE << (IOC_NR_BITS + IOC_TYPE_BITS + IOC_SIZE_BITS)
                      | DMA_BUF_SYNC_LAYOUT.size << (IOC_NR_BITS + IOC_TYPE_BITS)
                      | ord("b") << IOC_NR_BITS)
DMA_BUF_SYNC_WRITE = 2
DMA_BUF_SYNC_END = 4

INTROSPECTION_XML = """
<node>
  <interface name='com.vnpt.camera.Camera1'>
    <method name='StartStream'>
      <arg type='a{sv}' name='request' direction='in'/>
      <arg type='a{sv}' name='reply' direction='out'/>
    </method>
    <method name='StopStream'>
      <arg type='a{sv}' name='request' direction='in'/>
      <arg type='a{sv}' name='reply' direction='out'/>
    </method>
    <method name='GetStatus'>
      <arg type='a{sv}' name='request' direction='in'/>
      <arg type='a{sv}' name='reply' direction='out'/>
    </method>
  </interface>
</node>
"""


def parse_args():
    parser = argparse.ArgumentParser(description="Mock FW camera service for LACAI")
    parser.add_argument("--socket-dir", default=DEFAULT_SOCKET_DIR)
    parser.add_argument("--camera", type=int, default=0)
    parser.add_argument("--channel", type=int, default=0)
    parser.add_argument("--consumer", default="ai")
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--fps", type=int, default=30)
    parser.add_argument("--ack-timeout-s", type=float, default=15.0)
    parser.add_argument("--max-in-flight", type=int, default=3)
    parser.add_argument("--dma-heap", help="explicit DMA-BUF heap device for board fixture")
    parser.add_argument("--max-frames", type=int, default=0,
                        help="stop after N frames (0 = unlimited)")
    args = parser.parse_args()
    if args.max_in_flight <= 0 or args.width <= 0 or args.height <= 0 or args.fps <= 0:
        parser.error("max-in-flight, width, height and fps must be positive")
    if args.width % 2 or args.height % 2:
        parser.error("NV12 width and height must be even")
    return args


class FramePool:
    """One fixed-size pool whose slots return only on the matching wire ACK."""

    def __init__(self, args):
        self.frame_size = args.width * args.height * 3 // 2
        self.dma_heap = args.dma_heap
        self.slots = []
        self.available = queue.Queue(maxsize=args.max_in_flight)
        self.ioctl = ctypes.CDLL(None, use_errno=True).ioctl
        self.ioctl.argtypes = [ctypes.c_int, ctypes.c_ulong, ctypes.c_void_p]
        self.ioctl.restype = ctypes.c_int
        heap_fd = -1
        try:
            if self.dma_heap:
                heap_fd = os.open(self.dma_heap, os.O_RDONLY | os.O_CLOEXEC)
            for index in range(args.max_in_flight):
                if heap_fd >= 0:
                    payload = bytearray(DMA_HEAP_ALLOC_LAYOUT.pack(
                        self.frame_size, 0, os.O_RDWR | os.O_CLOEXEC, 0))
                    self.vqec_vision_ai_tools_fwsim_ioctl_buffer(
                        heap_fd, DMA_HEAP_IOCTL_ALLOC, payload)
                    fd = DMA_HEAP_ALLOC_LAYOUT.unpack(payload)[1]
                else:
                    fd = os.memfd_create(f"fwsim_frame_{index}", os.MFD_CLOEXEC)
                    os.ftruncate(fd, self.frame_size)
                try:
                    mapping = mmap.mmap(fd, self.frame_size, flags=mmap.MAP_SHARED,
                                        prot=mmap.PROT_READ | mmap.PROT_WRITE)
                except BaseException:
                    os.close(fd)
                    raise
                self.slots.append((fd, mapping))
                self.available.put_nowait(index)
        except BaseException:
            self.vqec_vision_ai_tools_fwsim_close_pool()
            raise
        finally:
            if heap_fd >= 0:
                os.close(heap_fd)

    def vqec_vision_ai_tools_fwsim_close_pool(self):
        for fd, mapping in self.slots:
            mapping.close()
            os.close(fd)
        self.slots.clear()

    def vqec_vision_ai_tools_fwsim_acquire_slot(self):
        return self.available.get_nowait()

    def vqec_vision_ai_tools_fwsim_release_slot(self, index):
        self.available.put_nowait(index)

    def vqec_vision_ai_tools_fwsim_ioctl_buffer(self, fd, request, payload):
        pointer = ctypes.addressof(ctypes.c_char.from_buffer(payload))
        if self.ioctl(fd, request, pointer) != 0:
            error = ctypes.get_errno()
            raise OSError(error, os.strerror(error))

    def vqec_vision_ai_tools_fwsim_write_frame(self, index, pixels):
        fd, mapping = self.slots[index]
        if self.dma_heap:
            self.vqec_vision_ai_tools_fwsim_ioctl_buffer(
                fd, DMA_BUF_IOCTL_SYNC,
                bytearray(DMA_BUF_SYNC_LAYOUT.pack(DMA_BUF_SYNC_WRITE)))
        try:
            mapping[:] = pixels
        finally:
            if self.dma_heap:
                self.vqec_vision_ai_tools_fwsim_ioctl_buffer(
                    fd, DMA_BUF_IOCTL_SYNC,
                    bytearray(DMA_BUF_SYNC_LAYOUT.pack(DMA_BUF_SYNC_WRITE | DMA_BUF_SYNC_END)))
        return os.dup(fd)


class CameraPipeline:
    def __init__(self, args):
        self.args = args
        self.pipeline = None
        self.appsink = None
        self.pool = None

    def start(self):
        desc = (
            f"qtiqmmfsrc name=camsrc camera={self.args.camera} ! "
            f"video/x-raw,format=NV12,width={self.args.width},height={self.args.height},"
            f"framerate={self.args.fps}/1 ! videoconvert ! video/x-raw,format=NV12 ! "
            "appsink name=cap max-buffers=2 drop=true sync=false"
        )
        self.pipeline = Gst.parse_launch(desc)
        self.appsink = self.pipeline.get_by_name("cap")
        state = self.pipeline.set_state(Gst.State.PLAYING)
        if state == Gst.StateChangeReturn.FAILURE:
            raise RuntimeError("camera pipeline failed to start")

    def stop(self):
        if self.pool is not None:
            self.pool.vqec_vision_ai_tools_fwsim_close_pool()
            self.pool = None
        if self.pipeline is not None:
            self.pipeline.set_state(Gst.State.NULL)
            self.pipeline = None

    def next_fd(self, timeout_ns):
        if self.pool is None:
            self.pool = FramePool(self.args)
        try:
            slot = self.pool.vqec_vision_ai_tools_fwsim_acquire_slot()
        except queue.Empty:
            return None
        try:
            result = self.vqec_vision_ai_tools_fwsim_copy_frame(slot, timeout_ns)
        except BaseException:
            self.pool.vqec_vision_ai_tools_fwsim_release_slot(slot)
            raise
        if result is None:
            self.pool.vqec_vision_ai_tools_fwsim_release_slot(slot)
        return result

    def vqec_vision_ai_tools_fwsim_copy_frame(self, slot, timeout_ns):
        sample = self.appsink.emit("try-pull-sample", timeout_ns)
        if sample is None:
            return None
        buf = sample.get_buffer()
        ok, info = buf.map(Gst.MapFlags.READ)
        if not ok:
            return None
        frame_size = self.args.width * self.args.height * 3 // 2
        meta = GstVideo.buffer_get_video_meta(buf)
        if meta is None or meta.n_planes != 2:
            buf.unmap(info)
            return None
        packed = bytearray(frame_size)
        destination = 0
        is_valid = True
        try:
            for plane, rows in ((0, self.args.height), (1, self.args.height // 2)):
                offset = meta.offset[plane]
                stride = meta.stride[plane]
                if stride < self.args.width or offset + rows * stride > len(info.data):
                    is_valid = False
                    break
                for row in range(rows):
                    start = offset + row * stride
                    packed[destination:destination + self.args.width] = \
                        info.data[start:start + self.args.width]
                    destination += self.args.width
        finally:
            buf.unmap(info)
        if not is_valid:
            return None
        frame_fd = self.pool.vqec_vision_ai_tools_fwsim_write_frame(slot, packed)
        return frame_fd, frame_size, slot

    def vqec_vision_ai_tools_fwsim_reset_pool(self):
        if self.pool is not None:
            self.pool.vqec_vision_ai_tools_fwsim_close_pool()
            self.pool = None


class RawFrameProducer:
    """AF_UNIX SOCK_SEQPACKET frame server matching the FW raw-frame wire."""

    def __init__(self, args, camera):
        self.args = args
        self.camera = camera
        self.socket_path = os.path.join(
            args.socket_dir, f"{args.channel}_third_{args.consumer}.sock")
        self.server = None
        self.running = False
        self.thread = None
        self.buf_id = 0
        self.frames_sent = 0

    def start(self):
        os.makedirs(self.args.socket_dir, exist_ok=True)
        if os.path.exists(self.socket_path):
            os.unlink(self.socket_path)
        self.server = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        self.server.bind(self.socket_path)
        self.server.listen(1)
        self.running = True
        self.thread = threading.Thread(target=self._serve, daemon=True)
        self.thread.start()
        print(f"producer listening on {self.socket_path}", flush=True)

    def stop(self):
        self.running = False
        try:
            if self.server is not None:
                self.server.close()
        finally:
            if self.thread is not None and self.thread is not threading.current_thread():
                self.thread.join(timeout=THREAD_STOP_TIMEOUT_S)
            if os.path.exists(self.socket_path):
                os.unlink(self.socket_path)

    def _serve(self):
        self.server.settimeout(0.5)
        while self.running:
            try:
                client, _ = self.server.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            print("consumer connected", flush=True)
            try:
                self._stream(client)
            except (BrokenPipeError, ConnectionResetError, OSError):
                print("consumer disconnected", flush=True)
            finally:
                client.close()

    def _stream(self, client):
        width = self.args.width
        height = self.args.height
        alloc = width * height * 3 // 2
        # FW does not serialize send/ACK: up to the receiver's live-lease budget may be in
        # flight. A synchronous mock deadlocks because the consumer holds the previous frame
        # until its next submission. ACKs are drained on a separate thread.
        in_flight = {}
        lock = threading.Lock()
        stop = threading.Event()

        def ack_reader():
            client.settimeout(0.5)
            try:
                while not stop.is_set():
                    try:
                        ack = client.recv(RETURN_HEADER.size)
                    except socket.timeout:
                        continue
                    except OSError:
                        break
                    if len(ack) != RETURN_HEADER.size:
                        break
                    buf_id = RETURN_HEADER.unpack(ack)[0]
                    with lock:
                        slot = in_flight.pop(buf_id, None)
                    if slot is None:
                        print(f"invalid or duplicate ACK buf_id={buf_id}", file=sys.stderr,
                              flush=True)
                        break
                    self.camera.pool.vqec_vision_ai_tools_fwsim_release_slot(slot)
            finally:
                # Wake the producer loop when the consumer disappears. Otherwise a full
                # in-flight window can keep this connection alive forever and prevent the
                # listening socket from accepting the next LACAI process.
                stop.set()

        ack_thread = threading.Thread(target=ack_reader, daemon=True)
        ack_thread.start()
        try:
            while self.running and not stop.is_set():
                with lock:
                    window_full = len(in_flight) >= self.args.max_in_flight
                if window_full:
                    stop.wait(0.001)
                    continue
                got = self.camera.next_fd(Gst.SECOND)
                if got is None:
                    stop.wait(0.001)
                    continue
                fd, size, slot = got
                self.buf_id += 1
                pts_ns = time.monotonic_ns()
                header = FRAME_HEADER.pack(
                    self.buf_id, width, height, GST_VIDEO_FORMAT_NV12, 2,
                    0, width * height, 0, 0,               # offsets[0..3]
                    width, width, 0, 0,                    # strides[0..3]
                    size, 0, alloc,                        # size, mem_offset, mem_maxsize
                    pts_ns, 0, 1000000000 // self.args.fps)
                with lock:
                    in_flight[self.buf_id] = slot
                try:
                    fd_array = array.array("i", [fd])
                    client.sendmsg([header], [(socket.SOL_SOCKET, socket.SCM_RIGHTS, fd_array)])
                except BaseException:
                    with lock:
                        in_flight.pop(self.buf_id, None)
                    self.camera.pool.vqec_vision_ai_tools_fwsim_release_slot(slot)
                    raise
                finally:
                    os.close(fd)
                self.frames_sent += 1
                if self.args.max_frames and self.frames_sent >= self.args.max_frames:
                    self.running = False
                    break
        finally:
            stop.set()
            try:
                client.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            ack_thread.join()
            # A disconnected consumer may retain a duplicated FD. Do not recycle its
            # allocation into a new connection even if its ACK never arrived.
            self.camera.vqec_vision_ai_tools_fwsim_reset_pool()


class CameraControlService:
    def __init__(self, args, producer):
        self.args = args
        self.producer = producer
        self.handle = "mock-third-0"
        self.active = False

    def _reply(self, **fields):
        return {key: GLib.Variant("s", str(value)) for key, value in fields.items()}

    def start_stream(self, parameters):
        request = parameters.unpack()[0] if parameters is not None else {}
        consumer = request.get("consumer_id", self.args.consumer)
        stream_id = request.get("stream_id", "third")
        if not self.producer.running:
            self.producer.start()
        self.active = True
        print(f"StartStream consumer={consumer} stream={stream_id}", flush=True)
        return self._reply(code=CODE_OK, stream_handle=self.handle, codec=CODEC_RAW,
                           width=self.args.width, height=self.args.height, fps=self.args.fps)

    def stop_stream(self, parameters):
        self.active = False
        print("StopStream", flush=True)
        return self._reply(code=CODE_OK, stream_handle=self.handle)

    def get_status(self, parameters):
        return self._reply(code=CODE_OK, stream_handle=self.handle, codec=CODEC_RAW,
                           width=self.args.width, height=self.args.height, fps=self.args.fps,
                           state="running" if self.active else "idle")

    def on_method_call(self, _connection, _sender, _path, _interface, method, parameters,
                       invocation):
        try:
            if method == "StartStream":
                reply = self.start_stream(parameters)
            elif method == "StopStream":
                reply = self.stop_stream(parameters)
            elif method == "GetStatus":
                reply = self.get_status(parameters)
            else:
                invocation.return_error_literal(
                    Gio.DBusError, Gio.DBusError.UNKNOWN_METHOD, method)
                return
            invocation.return_value(GLib.Variant("(a{sv})", (reply,)))
        except Exception as error:  # noqa: BLE001
            import traceback
            traceback.print_exc()
            print(f"control error: {error}", file=sys.stderr, flush=True)
            invocation.return_dbus_error(DEFAULT_INTERFACE, repr(error))

    def register(self):
        node = Gio.DBusNodeInfo.new_for_xml(INTROSPECTION_XML)
        interface = node.lookup_interface(DEFAULT_INTERFACE)
        connection = Gio.bus_get_sync(Gio.BusType.SYSTEM, None)
        connection.register_object(DEFAULT_OBJECT_PATH, interface, self.on_method_call,
                                   None, None)
        Gio.bus_own_name_on_connection(connection, DEFAULT_BUS_NAME,
                                       Gio.BusNameOwnerFlags.NONE, None, None)
        print(f"owning {DEFAULT_BUS_NAME}", flush=True)


def main():
    args = parse_args()
    Gst.init(None)
    camera = CameraPipeline(args)
    camera.start()
    producer = RawFrameProducer(args, camera)
    producer.start()
    control = CameraControlService(args, producer)
    control.register()
    loop = GLib.MainLoop()
    signal.signal(signal.SIGINT, lambda *_args: GLib.idle_add(loop.quit))
    signal.signal(signal.SIGTERM, lambda *_args: GLib.idle_add(loop.quit))
    try:
        loop.run()
    except KeyboardInterrupt:
        pass
    finally:
        producer.stop()
        camera.stop()


if __name__ == "__main__":
    main()
