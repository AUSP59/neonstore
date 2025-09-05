# Zsh completion for neonstore
# To load: fpath+=( $(pwd)/tools/completion ); autoload -U compinit; compinit
# compdef neonstore
_arguments "*: :->subcmds"
case $state in
  subcmds)
    _values "neonstore commands"       help list-products list-orders search-products add-product remove-product set-price restock sell       import export backup restore snapshot diff patch redact migrate policy-check compact trash doctor repair-integrity       metrics serve report watch tx jsonrpc verify-all catalog license buildinfo env bench selftest lint stats
  ;;
esac
