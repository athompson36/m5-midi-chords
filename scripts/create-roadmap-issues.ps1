# Creates GitHub issues from docs/DEV_ROADMAP.md checklist (one issue per line item).
# Requires: GitHub CLI authenticated (`gh auth login`).
# Usage: pwsh -File scripts/create-roadmap-issues.ps1
# Optional: $env:GH_REPO = "owner/repo" (default: remote origin)

$ErrorActionPreference = "Stop"
$gh = "${env:ProgramFiles}\GitHub CLI\gh.exe"
if (-not (Test-Path $gh)) {
  Write-Error "gh not found at $gh. Install GitHub CLI or set path."
}

$repo = $env:GH_REPO
if (-not $repo) {
  $remote = (git remote get-url origin 2>$null)
  if ($remote -match "github\.com[:/]([^/]+/[^/.]+)") {
    $repo = $Matches[1] -replace "\.git$", ""
  }
}
if (-not $repo) {
  Write-Error "Could not detect repo. Set env GH_REPO=owner/repo"
}

$items = @(
  @{ m = "milestone-1"; t = "M1: Install GCC toolchain and pass native PlatformIO tests"; b = @"
## Roadmap
Milestone 1 - Foundation Stabilization

## Acceptance criteria
- `gcc` and `g++` available on PATH (or documented alternative)
- `python -m platformio test -e native` passes locally and in CI (when added)
- Note toolchain setup in README or docs
"@ },
  @{ m = "milestone-1"; t = "M1: Add tests for settings row navigation and wrap behavior"; b = @"
## Roadmap
Milestone 1 - Foundation Stabilization

## Acceptance criteria
- Unit tests cover moving selection up/down including wrap first/last row
- Tests run under `platformio test -e native`
"@ },
  @{ m = "milestone-1"; t = "M1: Add tests for dual-corner settings-entry gesture timing"; b = @"
## Roadmap
Milestone 1 - Foundation Stabilization

## Acceptance criteria
- Extract/test logic for two-finger left+right bottom zones and hold duration threshold
- Tests cover below-threshold (no open) and above-threshold (open settings)
"@ },
  @{ m = "milestone-1"; t = "M1: Refactor touch handling (main / settings / gesture modules)"; b = @"
## Roadmap
Milestone 1 - Foundation Stabilization

## Acceptance criteria
- Clear separation: main screen touch, settings screen touch, dual-corner gesture
- No behavior regression on CoreS3 build
"@ },
  @{ m = "milestone-1"; t = "M1: Add script helper for build, test, and upload"; b = @"
## Roadmap
Milestone 1 - Foundation Stabilization

## Acceptance criteria
- One entry point (e.g. `scripts/build.ps1` or Makefile) for build/test/upload
- Document commands in README
"@ },
  @{ m = "milestone-1"; t = "M1: Add CI job to compile CoreS3 firmware on push"; b = @"
## Roadmap
Milestone 1 - Foundation Stabilization

## Acceptance criteria
- Workflow runs `platformio run -e m5stack-cores3` on PR/push
- Failing build blocks merge (if branch protection enabled)
"@ },

  @{ m = "milestone-2"; t = "M2: Transport settings (tempo/BPM, strum mode, swing/humanize)"; b = @"
## Roadmap
Milestone 2 - Settings System Expansion

## Acceptance criteria
- New settings rows with NVS persistence
- Values applied by playback/sequencer code paths when implemented
"@ },
  @{ m = "milestone-2"; t = "M2: MIDI settings (thru, transpose, filter beyond channel)"; b = @"
## Roadmap
Milestone 2 - Settings System Expansion

## Acceptance criteria
- Document each option; wire to MIDI engine when M3 lands
- Channel/OMNI already partially present; extend as specified in roadmap
"@ },
  @{ m = "milestone-2"; t = "M2: UI settings (theme/accent, touch hold thresholds)"; b = @"
## Roadmap
Milestone 2 - Settings System Expansion

## Acceptance criteria
- Configurable hold thresholds for long-press behaviors
- Optional theme/accent (or defer with explicit issue note)
"@ },
  @{ m = "milestone-2"; t = "M2: Audio/dynamics settings (velocity curve, defaults)"; b = @"
## Roadmap
Milestone 2 - Settings System Expansion

## Acceptance criteria
- Velocity curve or preset levels stored in NVS
- Used by MIDI OUT when suggestion playback exists
"@ },
  @{ m = "milestone-2"; t = "M2: Factory reset in settings"; b = @"
## Roadmap
Milestone 2 - Settings System Expansion

## Acceptance criteria
- Clears NVS preferences to defaults with confirmation
"@ },
  @{ m = "milestone-2"; t = "M2: Version/build info row in settings"; b = @"
## Roadmap
Milestone 2 - Settings System Expansion

## Acceptance criteria
- Shows firmware version or git describe / build timestamp
"@ },
  @{ m = "milestone-2"; t = "M2: NVS key schema validation and migration"; b = @"
## Roadmap
Milestone 2 - Settings System Expansion

## Acceptance criteria
- Versioned namespace or schema revision key
- Migration path documented for adding fields
"@ },

  @{ m = "milestone-3"; t = "M3: MIDI IN parser and active-note tracking"; b = @"
## Roadmap
Milestone 3 - MIDI Engine Integration

## Acceptance criteria
- Handles note on/off on selected input(s) with correct channel filtering
"@ },
  @{ m = "milestone-3"; t = "M3: Chord detection from incoming notes"; b = @"
## Roadmap
Milestone 3 - MIDI Engine Integration

## Acceptance criteria
- Detects chord quality from held notes; stable against spurious releases
"@ },
  @{ m = "milestone-3"; t = "M3: Suggestion generation and ranking"; b = @"
## Roadmap
Milestone 3 - MIDI Engine Integration

## Acceptance criteria
- Produces ordered suggestions aligned with product behavior (see web app reference)
"@ },
  @{ m = "milestone-3"; t = "M3: MIDI OUT playback (channel + velocity from settings)"; b = @"
## Roadmap
Milestone 3 - MIDI Engine Integration

## Acceptance criteria
- Sends notes on configured channel with configured velocity rules
"@ },
  @{ m = "milestone-3"; t = "M3: MIDI IN channel filter (OMNI vs fixed)"; b = @"
## Roadmap
Milestone 3 - MIDI Engine Integration

## Acceptance criteria
- Honors `midiInChannel` from settings (0 = all channels)
"@ },
  @{ m = "milestone-3"; t = "M3: All-notes-off on mode transitions and errors"; b = @"
## Roadmap
Milestone 3 - MIDI Engine Integration

## Acceptance criteria
- No stuck notes when changing page/mode or on fault recovery
"@ },
  @{ m = "milestone-3"; t = "M3: Debug overlay for MIDI diagnostics"; b = @"
## Roadmap
Milestone 3 - MIDI Engine Integration

## Acceptance criteria
- Optional screen showing last N MIDI events in/out
"@ },

  @{ m = "milestone-4"; t = "M4: Finalize main page layout for CoreS3"; b = @"
## Roadmap
Milestone 4 - CoreS3 UX Completion

## Acceptance criteria
- Readable typography and spacing at device resolution and rotation
"@ },
  @{ m = "milestone-4"; t = "M4: Pages for history / diatonic / functional / related / chromatic"; b = @"
## Roadmap
Milestone 4 - CoreS3 UX Completion

## Acceptance criteria
- Parity with legacy round-display / encoder-era page model, using touch navigation
"@ },
  @{ m = "milestone-4"; t = "M4: Fast page cycling via BACK/FWD long-press"; b = @"
## Roadmap
Milestone 4 - CoreS3 UX Completion

## Acceptance criteria
- Documented gesture; no conflict with settings dual long-press
"@ },
  @{ m = "milestone-4"; t = "M4: Confirmation prompts for destructive actions"; b = @"
## Roadmap
Milestone 4 - CoreS3 UX Completion

## Acceptance criteria
- Factory reset and similar actions require explicit confirm
"@ },
  @{ m = "milestone-4"; t = "M4: Visual feedback for gesture hold progress"; b = @"
## Roadmap
Milestone 4 - CoreS3 UX Completion

## Acceptance criteria
- User sees progress toward long-press thresholds where applicable
"@ },
  @{ m = "milestone-4"; t = "M4: Tune hitboxes and debounce for fingertip use"; b = @"
## Roadmap
Milestone 4 - CoreS3 UX Completion

## Acceptance criteria
- Reliable taps on soft buttons; reduced accidental double triggers
"@ },
  @{ m = "milestone-4"; t = "M4: Usability pass (bright and dim lighting)"; b = @"
## Roadmap
Milestone 4 - CoreS3 UX Completion

## Acceptance criteria
- Notes captured in issue comments or short doc
"@ },

  @{ m = "milestone-5"; t = "M5: Hardware E2E checklist document"; b = @"
## Roadmap
Milestone 5 - Hardware E2E and Release

## Acceptance criteria
- Single checklist doc in repo; covers gestures, MIDI, persistence
"@ },
  @{ m = "milestone-5"; t = "M5: Execute CoreS3 gesture tests (single + multi-touch)"; b = @"
## Roadmap
Milestone 5 - Hardware E2E and Release

## Acceptance criteria
- Results recorded (pass/fail) against checklist
"@ },
  @{ m = "milestone-5"; t = "M5: Persistence tests (save, reboot, verify)"; b = @"
## Roadmap
Milestone 5 - Hardware E2E and Release

## Acceptance criteria
- NVS values survive power cycle
"@ },
  @{ m = "milestone-5"; t = "M5: MIDI interoperability (2+ controllers)"; b = @"
## Roadmap
Milestone 5 - Hardware E2E and Release

## Acceptance criteria
- Notes on devices tested and any quirks
"@ },
  @{ m = "milestone-5"; t = "M5: 30-minute stability soak test"; b = @"
## Roadmap
Milestone 5 - Hardware E2E and Release

## Acceptance criteria
- No crash/hang; memory stable enough for v0.1 scope
"@ },
  @{ m = "milestone-5"; t = "M5: Tag v0.1.0 and publish flashing instructions"; b = @"
## Roadmap
Milestone 5 - Hardware E2E and Release

## Acceptance criteria
- Git tag + release notes with flash steps
"@ },
  @{ m = "milestone-5"; t = "M5: Release notes - known issues and recovery"; b = @"
## Roadmap
Milestone 5 - Hardware E2E and Release

## Acceptance criteria
- Known issues list + recovery (erase flash, safe mode, etc.)
"@ }
)

$created = @()
foreach ($it in $items) {
  $tmp = New-TemporaryFile
  try {
    Set-Content -Path $tmp -Value $it.b -Encoding utf8
    $url = & $gh issue create -R $repo -t $it.t -l $it.m -F $tmp
    $created += $url
    Write-Host $url
  } finally {
    Remove-Item -Force $tmp -ErrorAction SilentlyContinue
  }
}

Write-Host "`nCreated $($created.Count) issues."
