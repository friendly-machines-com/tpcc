program ExplicitEnumValues;

type
  TWeird = (
    Middle := 5,
    Lowest := -2,
    Highest := 9,
    Back := 1,
    Alias := Highest
  );
  TIndex = (
    IndexTwo = 2,
    IndexThree,
    IndexFour
  );
  TCalculated = (
    Calculated := 8 + 8,
    CalculatedNext
  );
  TCharacter = (
    CharacterA := 'A',
    CharacterB
  );
  TIndexArray = array[TIndex] of LongInt;

const
  Values: TIndexArray = (20, 30, 40);

begin
  if Ord(Low(TWeird)) <> $fffffffe then
    Halt(1);
  if Ord(High(TWeird)) <> 9 then
    Halt(2);
  if Ord(Low(TIndex)) <> 2 then
    Halt(3);
  if Ord(High(TIndex)) <> 4 then
    Halt(4);
  if Values[IndexTwo] <> 20 then
    Halt(5);
  if Values[IndexFour] <> 40 then
    Halt(6);
  if Ord(High(TCalculated)) <> 17 then
    Halt(7);
  if Ord(High(TCharacter)) <> 66 then
    Halt(8)
end.
