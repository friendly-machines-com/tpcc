program CrossEnumUnnamedValue;

type
  TFirst = (FirstName := 1, ThirdName := 3);

const
  { The cast produces an Integer-shaped folded node internally, but its Pascal
    type remains TFirst and must not make it an integer enum initializer. }
  UnnamedFirst = TFirst(2);

type
  TSecond = (Wrong := UnnamedFirst);

begin
end.
