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

Pixels come from the real Qualcomm camera through qtiqmmfsrc. For a board test
this mock copies each NV12 frame into a memfd and sends that FD, so the FD is a
plain mappable file (not a vendor dma-buf). The wire, socket naming, lease and
ACK semantics match FW; the memory backing does not. This is a test aid, not a
product camera service.

Run on the target (root), then start the LACAI app against:
  raw_source socket  : <socket_dir>/0_third_ai.sock
  camera D-Bus       : com.vnpt.camera.Camera1 on the system bus
"""

import argparse
import array
import os
import socket
import struct
import sys
import threading

import gi

gi.require_version("Gst", "1.0")
gi.require_version("Gio", "2.0")
from gi.repository import Gio, GLib, Gst  # noqa: E402

FRAME_HEADER = struct.Struct("<QIIII4I4iQQQQQQ")
RETURN_HEADER = struct.Struct("<Q")
GST_VIDEO_FORMAT_NV12 = 23
DEFAULT_SOCKET_DIR = "/run/camera_ai"
DEFAULT_BUS_NAME = "com.vnpt.camera.Camera"
DEFAULT_OBJECT_PATH = "/com/vnpt/camera/Camera"
DEFAULT_INTERFACE = "com.vnpt.camera.Camera1"
CODE_OK = 0
CODEC_RAW = "RAW"

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
    parser.add_argument("--max-frames", type=int, default=0,
                        help="stop after N frames (0 = unlimited)")
    return parser.parse_args()


class CameraPipeline:
    def __init__(self, args):
        self.args = args
        self.pipeline = None
        self.appsink = None
        self.memfd = -1

    def start(self):
        desc = (
            f"qtiqmmfsrc name=camsrc camera={self.args.camera} ! "
            f"video/x-raw,format=NV12,width={self.args.width},height={self.args.height},"
            f"framerate={self.args.fps}/1 ! videoconvert ! video/x-raw,format=NV12 ! "
            "appsink name=cap max-buffers=2 drop=true sync=false"
        )
        self.pipeline = Gst.parse_launch(desc)
        self.appsink = self.pipeline.get_by_name("cap")
        self.memfd = os.memfd_create("fwsim_frame", 0)
        state = self.pipeline.set_state(Gst.State.PLAYING)
        if state == Gst.StateChangeReturn.FAILURE:
            raise RuntimeError("camera pipeline failed to start")

    def stop(self):
        if self.pipeline is not None:
            self.pipeline.set_state(Gst.State.NULL)
            self.pipeline = None
        if self.memfd >= 0:
            os.close(self.memfd)
            self.memfd = -1

    def next_fd(self, timeout_ns):
        sample = self.appsink.emit("try-pull-sample", timeout_ns)
        if sample is None:
            return None
        buf = sample.get_buffer()
        ok, info = buf.map(Gst.MapFlags.READ)
        if not ok:
            return None
        data = bytes(info.data)
        buf.unmap(info)
        if len(data) < self.args.width * self.args.height * 3 // 2:
            return None
        os.ftruncate(self.memfd, len(data))
        os.pwrite(self.memfd, data, 0)
        return self.memfd, len(data)


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
        client.settimeout(2.0)
        while self.running:
            got = self.camera.next_fd(Gst.SECOND)
            if got is None:
                continue
            fd, size = got
            self.buf_id += 1
            header = FRAME_HEADER.pack(
                self.buf_id, width, height, GST_VIDEO_FORMAT_NV12, 2,
                0, width * height, 0, 0,               # offsets[0..3]
                width, width, 0, 0,                    # strides[0..3]
                size, 0, alloc,                        # size, mem_offset, mem_maxsize
                self.buf_id * 1000000000 // self.args.fps, 0, 1000000000 // self.args.fps)
            fd_array = array.array("i", [fd])
            client.sendmsg([header], [(socket.SOL_SOCKET, socket.SCM_RIGHTS, fd_array)])
            ack = client.recv(RETURN_HEADER.size)
            if len(ack) != RETURN_HEADER.size:
                raise ConnectionResetError("missing return ACK")
            self.frames_sent += 1
            if self.args.max_frames and self.frames_sent >= self.args.max_frames:
                self.running = False
                break


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
    try:
        loop.run()
    except KeyboardInterrupt:
        pass
    finally:
        producer.stop()
        camera.stop()


if __name__ == "__main__":
    main()
