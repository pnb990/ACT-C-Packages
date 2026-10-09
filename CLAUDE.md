<!--
SPDX-FileCopyrightText: 2026 Pierre-Noel Bouteville <pnb990@gmail.com>

SPDX-License-Identifier: BSD-3-Clause
-->

# ACT-C-Packages: notes for Claude

## Copyright and licence headers

Every file carries a REUSE header; `lint-reuse` fails the CI on one without.
Both licence texts are in `LICENSES/`: `LicenseRef-ALLCircuits-ACT-1.1` and
`BSD-3-Clause`.

The address that counts is the committer's, `git config user.email`.

- **A file added** gets the committer's copyright line. Its licence depends on
  that address:
  - an `@allcircuits.com` address: `LicenseRef-ALLCircuits-ACT-1.1`;
  - any other address: `BSD-3-Clause`.

  ```
  SPDX-FileCopyrightText: <year> <committer name> <committer email>

  SPDX-License-Identifier: BSD-3-Clause
  ```

  A file that cannot hold a comment (JSON, for instance) takes the header in a
  `<file>.license` next to it.
- **An existing file modified** keeps its licence and its copyright lines. If
  the committer's address is not among them, a line for it is added under the
  existing ones:

  ```
  SPDX-FileCopyrightText: 2026 Pierre-Noel Bouteville <pierre-noel.bouteville@allcircuits.com>
  SPDX-FileCopyrightText: <year> <committer name> <committer email>

  SPDX-License-Identifier: LicenseRef-ALLCircuits-ACT-1.1
  ```

  If it is already there, nothing is added; a change in a later year extends
  that line's year (`2026-2027`).
