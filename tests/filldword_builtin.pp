program FillDWordBuiltin;

type
  TDWords = array[0..4] of DWord;
  PDWords = ^TDWords;

var
  Values: TDWords;
  ValuesPointer: PDWords;

begin
  Values[0] := 7;
  Values[4] := 9;
  FillDWord(Values[1], 3, DWord($12345678));
  if Values[0] <> 7 then
    Halt(1);
  if Values[1] <> DWord($12345678) then
    Halt(2);
  if Values[2] <> DWord($12345678) then
    Halt(3);
  if Values[3] <> DWord($12345678) then
    Halt(4);
  if Values[4] <> 9 then
    Halt(5);

  FillDWord(Values[1], 0, 0);
  FillDWord(Values[1], -1, 0);
  if Values[1] <> DWord($12345678) then
    Halt(6);

  ValuesPointer := @Values;
  FillDWord(ValuesPointer^, 5, LongWord(-1));
  if Values[0] <> LongWord(-1) then
    Halt(7);
  if Values[2] <> LongWord(-1) then
    Halt(8);
  if Values[4] <> LongWord(-1) then
    Halt(9)
end.
