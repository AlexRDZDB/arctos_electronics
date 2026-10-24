#!/usr/bin/env bash
# Starts the container, builds the workspace, runs ArduinoTestNode,
# sends 1 (go to 180 deg), waits ON_SECONDS, sends 0 (back to 0 deg), then stops the node.
#
# Usage: ./run_test.sh [on_seconds]   (default 5)
set -euo pipefail

ON_SECONDS="${1:-5}"
CONTAINER=arctos_arduino
cd "$(dirname "$0")"

echo "==> Starting container"
docker compose up -d

echo "==> Building workspace"
docker exec "$CONTAINER" bash -c "source /opt/ros/jazzy/setup.bash && cd /ros_ws && colcon build --symlink-install" >/dev/null

docker exec -i -e ON_SECONDS="$ON_SECONDS" "$CONTAINER" bash -s <<'EOF'
set -e
source /opt/ros/jazzy/setup.bash
source /ros_ws/install/setup.bash
LOG=/tmp/arduino_test_node.log

# Only one process can hold the serial port
if pgrep -x ArduinoTestNode >/dev/null; then
    echo "==> Stopping already-running ArduinoTestNode"
    pkill -TERM -x ArduinoTestNode || true
    sleep 2
fi

echo "==> Starting ArduinoTestNode on ${ARDUINO_PORT}"
ros2 run arduino_test ArduinoTestNode > "$LOG" 2>&1 &
NODE_PID=$!
tail -n +1 -f "$LOG" &
TAIL_PID=$!
# Background jobs in a non-interactive shell ignore SIGINT, so stop the node with SIGTERM
trap 'pkill -TERM -x ArduinoTestNode; wait $NODE_PID 2>/dev/null; kill $TAIL_PID 2>/dev/null' EXIT

# Wait for the serial connection (the Nano resets when the port opens)
for _ in $(seq 1 30); do
    grep -q "Connected to Arduino" "$LOG" && break
    if ! kill -0 $NODE_PID 2>/dev/null; then echo "Node exited"; exit 1; fi
    sleep 0.5
done
grep -q "Connected to Arduino" "$LOG" || { echo "==> Could not connect to ${ARDUINO_PORT}"; exit 1; }
sleep 1   # let the sketch finish setup()

echo "==> 1: go to 180 deg, hold ${ON_SECONDS}s"
ros2 topic pub --once -w 1 /on_off std_msgs/msg/Int32 "{data: 1}" >/dev/null
sleep "$ON_SECONDS"

echo "==> 0: back to 0 deg"
ros2 topic pub --once -w 1 /on_off std_msgs/msg/Int32 "{data: 0}" >/dev/null
sleep 3   # allow Arduino replies to be logged

echo "==> Done"
EOF
