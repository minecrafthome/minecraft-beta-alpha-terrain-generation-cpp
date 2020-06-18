# How to Contribute

We'd love to accept your contributions to this project. Here are a few small guidelines you must follow.

## Commit Messages

We follow the [Conventional Commits specification][conventional-commits] to help keep the commit history readable and to automate release process & changelogs.

### Commit Message Format

The commit messages should be lower-case and follow this format:

```text
<type>[optional scope]: <description>

[optional body]

[optional footer(s)]
```

We allow the _type_ `chore`, `fix`, `feat`, and BREAKING CHANGE only:

- **chore:** a commit of the _type_ `chore` indicates a change which has no effect on the end user (this type does not increment the version).
- **fix:** a commit of the _type_ `fix` patches a bug in your codebase (this correlates with [`PATCH`](http://semver.org/#summary) in semantic versioning).
- **feat:** a commit of the _type_ `feat` introduces a new feature to the codebase (this correlates with [`MINOR`](http://semver.org/#summary) in semantic versioning).
- **BREAKING CHANGE:** a commit that appends a `!` after the type/scope and optionally has a footer `BREAKING CHANGE:`, introduces a breaking change (correlating with [`MAJOR`](http://semver.org/#summary) in semantic versioning).

Commits of any _type_ can be a BREAKING CHANGE.

Refer to the [specification][conventional-commits] for more information.

## Code Reviews

All submissions, including submissions by project members, require review.

[conventional-commits]: https://www.conventionalcommits.org/en/v1.0.0/
