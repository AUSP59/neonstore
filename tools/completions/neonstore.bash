_neonstore_complete() {
  local cur prev
  COMPREPLY=()
  cur="${COMP_WORDS[COMP_CWORD]}"
  prev="${COMP_WORDS[COMP_CWORD-1]}"
  case "$prev" in
    add-product) COMPREPLY=( $(compgen -W "--id --name --price --stock" -- "$cur") ); return ;;
    restock) COMPREPLY=( $(compgen -W "--id --qty" -- "$cur") ); return ;;
    set-price) COMPREPLY=( $(compgen -W "--id --price" -- "$cur") ); return ;;
    remove-product) COMPREPLY=( $(compgen -W "--id" -- "$cur") ); return ;;
    find-product) COMPREPLY=( $(compgen -W "--id --output" -- "$cur") ); return ;;
    export) COMPREPLY=( $(compgen -W "--format --out" -- "$cur") ); return ;;
    save|load|validate) COMPREPLY=( $(compgen -W "--dir" -- "$cur") ); return ;;
    list-products) COMPREPLY=( $(compgen -W "--output --backend --dir --dsn --config" -- "$cur") ); return ;;
    *) COMPREPLY=( $(compgen -W "add-product list-products restock set-price remove-product find-product export save load stats health validate --help --version --backend --dir --dsn --config --no-color --verbose" -- "$cur") );;
  esac
}
complete -F _neonstore_complete neonstore

# Updated options for new commands
complete -o filenames -F _neonstore_complete neonstore
