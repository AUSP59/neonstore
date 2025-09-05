# PowerShell tab completion for neonstore
Register-ArgumentCompleter -CommandName neonstore -ScriptBlock {
  param($wordToComplete, $commandAst, $cursorPosition)
  $cmds = @(
    "help","list-products","search-products","list-orders","add-product","remove-product","set-price","restock","sell",
    "import","export","backup","restore","snapshot","diff","patch","redact","migrate","policy-check","metrics","serve",
    "report","compact","trash","doctor","repair-integrity","verify-all","catalog","license","buildinfo","jsonrpc","tx","watch","archive","dataset","env"
  )
  $cmds | Where-Object { $_ -like "$wordToComplete*" } | ForEach-Object { [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_) }
}
