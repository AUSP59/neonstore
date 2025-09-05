_neonstore_complete() {
  local cur prev
  COMPREPLY=()
  cur="${COMP_WORDS[COMP_CWORD]}"
  prev="${COMP_WORDS[COMP_CWORD-1]}"
  local cmds="help version verify-all products orders search csv policy merge vdiff normalize backup snapshot report health fuzz lint audit ledger config sbom doctor generate dedupe seal analytics"
  if [[ ${COMP_CWORD} -le 1 ]]; then
    COMPREPLY=( $(compgen -W "${cmds}" -- "$cur") )
    return 0
  fi
}
complete -F _neonstore_complete neonstore
