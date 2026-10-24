FROM osrf/ros:jazzy-desktop-full

ENV DEBIAN_FRONTEND=noninteractive

# pyserial for talking to the Arduino over USB serial
RUN apt-get update && apt-get install -y --no-install-recommends \
        python3-serial \
        python3-colcon-common-extensions \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /ros_ws

# Source ROS (and the workspace once built) in every interactive shell
RUN echo "source /opt/ros/jazzy/setup.bash" >> /root/.bashrc && \
    echo "[ -f /ros_ws/install/setup.bash ] && source /ros_ws/install/setup.bash" >> /root/.bashrc

CMD ["sleep", "infinity"]
