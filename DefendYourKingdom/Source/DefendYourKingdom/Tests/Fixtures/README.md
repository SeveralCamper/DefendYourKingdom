# Legacy profile fixture

`ProfileSchema1.sav` is the original UE5.5 schema-1 snapshot created during the
M1.01 cold-start check, before M1.03 added the command ledger and two-copy container.
It was copied unchanged from `Saved/M101/fresh/Saved/SaveGames/DYK_LocalProfile.sav`.
It contains the empty initial local profile, with no account or personal data.

- Size: 1739 bytes.
- SHA-256: `55D730A617DA8CD4D3ED9488B966E064DD302FDD8CC48EEC915F45B17A0DAF7C`.
- `DYK.M103.RealLegacyFixtureMigration` checks migration of this historical file.
- The populated synthetic legacy test separately checks preservation of every
  profile field. Test values are fixtures, not game balance.
