program SetLiterals;

type
  TIntegerSet = set of Integer;
  TCharSet = set of Char;
  TColour = (Red, Green, Blue);
  TColourSet = set of TColour;

var
  Integers: TIntegerSet;
  Characters: TCharSet;
  Colours: TColourSet;
  IntegerHit: Boolean;
  IntegerMiss: Boolean;
  RangeHit: Boolean;
  EmptyMiss: Boolean;
  TabHit: Boolean;
  LetterHit: Boolean;
  CharacterMiss: Boolean;
  EnumHit: Boolean;
  EnumMiss: Boolean;
  DirectHit: Boolean;
  DirectEmptyMiss: Boolean;
  IntegerProbe: Integer;
  NarrowSetHit: Boolean;
  NarrowSetHighMiss: Boolean;
  NarrowSetLowMiss: Boolean;

begin
  Integers := [1, 3..5];
  IntegerHit := 1 in Integers;
  IntegerMiss := 2 in Integers;
  RangeHit := 4 in Integers;
  Integers := [];
  EmptyMiss := 1 in Integers;

  Characters := [#9, ' ', 'A'..'C'];
  TabHit := #9 in Characters;
  LetterHit := 'B' in Characters;
  CharacterMiss := 'D' in Characters;

  Colours := [Red, Blue];
  EnumHit := Blue in Colours;
  EnumMiss := Green in Colours;

  DirectHit := 5 in [4, 5];
  DirectEmptyMiss := 1 in [];

  {$R+}
  IntegerProbe := 128;
  NarrowSetHit :=
    IntegerProbe in [1,2,4,8,16,32,64,128];
  IntegerProbe := 256;
  NarrowSetHighMiss :=
    IntegerProbe in [1,2,4,8,16,32,64,128];
  IntegerProbe := -1;
  NarrowSetLowMiss :=
    IntegerProbe in [1,2,4,8,16,32,64,128]
end.
