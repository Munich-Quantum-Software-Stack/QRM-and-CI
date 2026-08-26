export LD_LIBRARY_PATH=/workspace/build/_deps/qdmi-build/examples/driver:/workspace/build/_deps/qdmi-build/examples/device:$LD_LIBRARY_PATH
export QDMI_CONF=/workspace/build/deployment/workflow-aio/qdmi.conf

echo "/workspace/build/_deps/qdmi-build/examples/device/libcxx_device.so CXX" > "${QDMI_CONF}"

# ConfigDefaults.hpp.in defaults QRM_AMQP_HOST to host.docker.internal (a
# RabbitMQ running on the Docker host), but our dev workflow runs RabbitMQ
# inside this same container (`sudo service rabbitmq-server start`), so
# point at it locally instead.
export QRM_AMQP_HOST=localhost

# Runners.cpp's compile step shells out to `mqss-opt`, which isn't installed
# anywhere on PATH by default - it only exists inside mqssci's own build tree.
export PATH=/workspace/build/_deps/mqssci-src/build/bin:$PATH