"""ArduinoTestNode: forwards Int32 messages on /on_off to an Arduino over serial.

Each received value is written as an ASCII line, e.g. "1\n" or "0\n".
"""
import os
import time

import rclpy
from rclpy.executors import ExternalShutdownException
from rclpy.node import Node
from std_msgs.msg import Int32
import serial


class ArduinoTestNode(Node):

    def __init__(self):
        super().__init__('ArduinoTestNode')

        self.declare_parameter('port', os.environ.get('ARDUINO_PORT', '/dev/ttyUSB0'))
        self.declare_parameter('baud', 115200)
        self.port = self.get_parameter('port').value
        self.baud = self.get_parameter('baud').value

        self.ser = None
        self.connect()
        # Keep retrying if the Arduino is unplugged / not yet connected
        self.create_timer(2.0, self.check_connection)

        self.create_subscription(Int32, 'on_off', self.on_off_callback, 10)
        self.get_logger().info(f'Subscribed to /on_off, forwarding to {self.port} @ {self.baud}')

    def connect(self):
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=0.1)
            # Opening the port resets the Nano; give the bootloader time to finish.
            # Startup output from setup() is kept so it shows up in the log.
            time.sleep(2.0)
            self.get_logger().info(f'Connected to Arduino on {self.port}')
        except serial.SerialException as e:
            self.ser = None
            self.get_logger().warn(f'Could not open {self.port}: {e}', throttle_duration_sec=10.0)

    def check_connection(self):
        if self.ser is None:
            self.connect()
            return
        # Print anything the Arduino sends back (status/debug lines)
        try:
            while self.ser.in_waiting:
                line = self.ser.readline().decode(errors='replace').strip()
                if line:
                    self.get_logger().info(f'[Arduino] {line}')
        except (serial.SerialException, OSError) as e:
            self.get_logger().error(f'Serial error: {e}')
            self.ser = None

    def on_off_callback(self, msg):
        if self.ser is None:
            self.get_logger().warn(f'Arduino not connected, dropping value {msg.data}')
            return
        try:
            self.ser.write(f'{msg.data}\n'.encode())
            self.get_logger().info(f'Sent {msg.data} to Arduino')
        except (serial.SerialException, OSError) as e:
            self.get_logger().error(f'Serial write failed: {e}')
            self.ser = None


def main(args=None):
    rclpy.init(args=args)
    node = ArduinoTestNode()
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    finally:
        if node.ser is not None:
            node.ser.close()
        node.destroy_node()
        rclpy.try_shutdown()


if __name__ == '__main__':
    main()
