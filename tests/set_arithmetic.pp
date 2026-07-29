program SetArithmetic;

type
  TCharSet = set of Char;
  TByteSet = set of Byte;
  TWordSet = set of Word;
  TMarker = (
    MarkerA, MarkerB, MarkerC, MarkerD);
  TMarkerSet = set of TMarker;

const
  ConstantFirst = [MarkerA, MarkerB];
  ConstantSecond = [MarkerB..MarkerD];
  ConstantUnion = ConstantFirst + ConstantSecond;
  ConstantDifference = ConstantUnion - [MarkerB..MarkerC];

var
  Separators: TCharSet;
  Characters: TCharSet;
  Bytes: TByteSet;
  Words: TWordSet;
  Markers: TMarkerSet;
  Selected: Integer;

operator UncheckedAdd(
  const First, Second: TMarkerSet): TMarkerSet;
begin
  Selected := 1;
  Result := First;
  if MarkerB in Second then
    Include(Result, MarkerB);
  Include(Result, MarkerC)
end;

operator Add(
  const First, Second: TMarkerSet): TMarkerSet;
begin
  Selected := 2;
  Result := First;
  if MarkerB in Second then
    Include(Result, MarkerB);
  Include(Result, MarkerD)
end;

begin
  Separators := [' ', '''', '"'];
  {$Q-}
  Separators := Separators + [#13, #10] - ['''', '"'];
  if not (#10 in Separators) or
     not (#13 in Separators) or
     not (' ' in Separators) or
     ('''' in Separators) or
     ('"' in Separators) then
    Halt(1);

  {$Q+}
  Characters := ['a'..'d'] + ['d'..'f'];
  if not ('a' in Characters) or
     not ('f' in Characters) then
    Halt(2);
  Characters := Characters - ['b'..'e'];
  if not ('a' in Characters) or
     not ('f' in Characters) or
     ('c' in Characters) then
    Halt(3);

  { Two uncommitted bracket literals infer one candidate-local set type. }
  Characters := ['x'] + ['y'];
  if not ('x' in Characters) or
     not ('y' in Characters) then
    Halt(4);

  { The wider set domain is the result when one set domain contains another. }
  Bytes := [1, 2];
  Words := [];
  Include(Words, 2);
  Include(Words, 300);
  Words := Bytes + Words;
  if not (1 in Words) or
     not (300 in Words) then
    Halt(5);

  { A typed custom declaration is an ordinary, better overload than the
    generic System relation, under both checked identities. }
  {$Q-}
  Selected := 0;
  Markers := [MarkerA];
  Markers := Markers + [MarkerB];
  if (Selected <> 1) or
     not (MarkerC in Markers) then
    Halt(6);
  {$Q+}
  Selected := 0;
  Markers := Markers + [MarkerB];
  if (Selected <> 2) or
     not (MarkerD in Markers) then
    Halt(7);

  if not (MarkerA in ConstantUnion) or
     not (MarkerD in ConstantUnion) then
    Halt(8);
  if not (MarkerA in ConstantDifference) or
     not (MarkerD in ConstantDifference) or
     (MarkerB in ConstantDifference) or
     (MarkerC in ConstantDifference) then
    Halt(9)
end.
