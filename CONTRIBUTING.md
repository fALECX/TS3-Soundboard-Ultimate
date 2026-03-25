# Contributing

Thanks for contributing to RP Soundboard Ultimate.

## Workflow

1. Fork and create a feature branch.
2. Keep changes focused and small.
3. Run checks locally:
   - `npm ci`
   - `npm run dev`
   - `npm run dist` (for packaging-related changes)
4. Update docs/changelog when behavior changes.
5. Open a PR with:
   - problem statement
   - approach summary
   - test evidence (logs/screenshots)

## Contribution Standards

- Keep IPC channel compatibility unless a breaking change is intentional.
- Do not commit copyrighted audio samples.
- Prefer clear code, minimal side effects, and explicit error handling.

## Pull Request Checklist

- [ ] Build still works from clean install
- [ ] Runtime config/sounds persist correctly
- [ ] README/changelog updated if needed
- [ ] No secrets, API keys, or local artifacts committed