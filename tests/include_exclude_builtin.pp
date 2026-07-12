program IncludeExcludeBuiltin;

type
  TIndex = 10..20;
  TIndexSet = set of TIndex;
  TCharSet = set of Char;

var
  Numbers: TIndexSet;
  Characters: TCharSet;
  TenPresent: Boolean;
  ElevenPresent: Boolean;
  TwelvePresent: Boolean;
  ThirteenPresent: Boolean;
  FifteenPresent: Boolean;
  NulPresent: Boolean;
  APresent: Boolean;

begin
  Numbers := [10..12, 15];
  Include(Numbers, 13);
  Include(Numbers, 12);
  Exclude(Numbers, 11);
  Exclude(Numbers, 12);
  Exclude(Numbers, 15);

  TenPresent := 10 in Numbers;
  ElevenPresent := 11 in Numbers;
  TwelvePresent := 12 in Numbers;
  ThirteenPresent := 13 in Numbers;
  FifteenPresent := 15 in Numbers;

  Characters := [];
  Include(Characters, #0);
  Include(Characters, 'A');
  Exclude(Characters, #0);
  NulPresent := #0 in Characters;
  APresent := 'A' in Characters
end.
