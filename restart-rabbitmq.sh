if command -v sudo >/dev/null 2>&1; then
  sudo service rabbitmq-server restart
else
  service rabbitmq-server restart
fi
