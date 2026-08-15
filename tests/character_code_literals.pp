program CharacterCodeLiterals;

const
  DecimalA = #65;
  HexadecimalA = #$41;
  Mixed = 'A'#$00'B';
  LowercaseDigits = #$af;
  AlignmentBytes =
    #$66#$66#$66#$0F#$1F#$84#$00#$00#$00#$00#$00;

var
  Character: Char;
  Text: ShortString;

begin
  if HexadecimalA <> DecimalA then
    Halt(1);

  Character := #$FF;
  if Ord(Character) <> 255 then
    Halt(2);
  if Ord(LowercaseDigits) <> $AF then
    Halt(3);

  Text := Mixed;
  if Length(Text) <> 3 then
    Halt(4);
  if (Text[1] <> 'A') or
     (Ord(Text[2]) <> 0) or
     (Text[3] <> 'B') then
    Halt(5);

  Text := AlignmentBytes;
  if Length(Text) <> 11 then
    Halt(6);
  if (Ord(Text[1]) <> $66) or
     (Ord(Text[4]) <> $0F) or
     (Ord(Text[6]) <> $84) or
     (Ord(Text[11]) <> 0) then
    Halt(7)
end.
