from setuptools import setup

package_name = 'arduino_test'

setup(
    name=package_name,
    version='0.1.0',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages', ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='alexpurdue',
    maintainer_email='alejandrordzdb@gmail.com',
    description='Bridges a ROS 2 Int32 topic to an Arduino Nano over USB serial.',
    license='MIT',
    entry_points={
        'console_scripts': [
            'ArduinoTestNode = arduino_test.arduino_test_node:main',
        ],
    },
)
