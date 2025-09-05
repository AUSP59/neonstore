# Retention

- **Audit logs**: rotate with `audit rotate --max-bytes B --keep K`, and prune by age with `retention enforce --audit --days D`.
- **Backups**: prune aged artifacts with `retention enforce --backups --days D`.
- **Trash**: purge aged trash with `trash purge --age-days D`.
