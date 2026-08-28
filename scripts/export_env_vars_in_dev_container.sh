export LD_LIBRARY_PATH=/workspace/build/_deps/qdmi-build/examples/driver:/workspace/build/_deps/qdmi-build/examples/device:$LD_LIBRARY_PATH
export QDMI_CONF=/workspace/build/apps/standalone/qdmi.conf

echo "/workspace/build/_deps/qdmi-build/examples/device/libcxx_device.so CXX" > "${QDMI_CONF}"

# include/qrmci/ConfigDefaults.h.in defaults QRM_AMQP_HOST (AMPQHost) to
# host.docker.internal (a RabbitMQ running on the Docker host), but our dev
# workflow runs RabbitMQ inside this same container
# (`sudo service rabbitmq-server start`), so point at it locally instead.
export QRM_AMQP_HOST=localhost
