export PASSES="$HOME/bin/lib/passes"
export QDMI_CONFIG_FILE="$HOME/qrm.git/.qdmi-config"
export CONF_IBM="$HOME/qrm.git/inputs/conf.json"
export TOKEN_WMI="$HOME/qrm.git/tokens/token_wmi.txt"
export TOKEN_PLANQC="$HOME/qrm.git/tokens/token_planqc.txt"
make test_wmi_real
