# Bash completion for neonstore
_neonstore_complete() {
  local cur prev opts
  COMPREPLY=()
  cur="${COMP_WORDS[COMP_CWORD]}"
  opts="help list-products list-orders search-products add-product remove-product set-price restock sell import export backup restore snapshot diff patch redact migrate policy-check compact trash doctor repair-integrity metrics serve report watch tx jsonrpc verify-all catalog license buildinfo env bench selftest lint stats"
  COMPREPLY=( $(compgen -W "${opts}" -- ${cur}) )
  return 0
}
complete -F _neonstore_complete neonstore
