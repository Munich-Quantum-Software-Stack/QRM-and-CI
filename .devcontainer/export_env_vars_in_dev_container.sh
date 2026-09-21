export LD_LIBRARY_PATH=/workspace/build/_deps/qdmi-build/examples/driver:/workspace/build/_deps/qdmi-build/examples/device/src:$LD_LIBRARY_PATH
export QDMI_CONF=/workspace/build/apps/standalone/qdmi.conf

echo "/workspace/build/_deps/qdmi-build/examples/device/src/libcxx-qdmi-device.so CXX" > "${QDMI_CONF}"

# The compiled-in AMQPHost default (include/qrmci/ConfigDefaults.h.in) is
# 127.0.0.1, which a container resolves to itself. That happens to be right
# here -- the dev workflow runs RabbitMQ inside this same container
# (`sudo service rabbitmq-server start`) -- but set it explicitly so the
# intent survives a change to that default.
export QRMCI_AMQP_HOST=localhost
