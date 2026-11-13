# Aces High film companion - to do

The one list. Update it with every change; the manual (MANUAL.md) changes with every feature in the same commit.

## Next
- [ ] A picture of the kill-sequence menu for the manual (manual_shots.py can't open a popup offscreen yet).
- [ ] Undo for shot changes (a delete or Retake by mistake).
- [ ] Shots dragged on the timeline (move and stretch the olive bars), as well as the ◂ ▸ − + buttons.
- [ ] The tablet page brought up to date with the window (shot timing, timeline bars and drops, save/load, kill
      sequence, Browse) - while it ships switched off, before it's switched on.
- [ ] More kinds from the director's playbook as one-click recipes: the fly-by, gear cameras for landings, the bomb run.
- [x] Packaging: a zip of build/dist named with the version, for the release page (package.sh, 12 Nov 2026: build/aces-high-film-companion-<ver>-win64.zip + .sha256; version from CMakeLists project()).

## Waiting on others
- [ ] **Code signing:** SignPath Foundation turned it down on 13 Nov 2026 - the project is too new. Releases stay
      unsigned (with a published SHA-256). Options: reapply to SignPath once it has a track record; Certum's
      open-source certificate (paid, ID check); Azure Artifact Signing (check individual eligibility). When one is in
      place: signing step in build.yml (token as a repo secret), README policy wording, a signed release from CI.
- [ ] **Announcing it:** HiTech has no issue with distribution but is deciding about an unsigned exe on his site.
- [ ] **Tablet page:** off unless started with `--tablet`, until HiTech gives his view (hitech repo direct/NEXT.md).

## Done
- [x] **0.1.0 released** (13 Nov 2026): repo public, GitHub release v0.1.0 with the zip. Steely published it so HiTech
      could see it (he'd asked to); his PM answer to the release question not yet in the hitech repo.
- [x] v1: viewer link, camera rig and sliders, live preview, shots, render; docking; studio look and icon; packaging;
      manual (MD + HTML with pictures); film picker; default output folder; shot timing; timeline shot bars and drops;
      save/load/clear shots; kill sequence; finding the viewer; render timer (12 Nov 2026).
- [x] Build 9 (12 Nov 2026): AHF-30 fixed, so the kill sequence uses the recorded cockpit again and the manual's note is
      gone; AHF-28 fixed (the shimmer note gone).
