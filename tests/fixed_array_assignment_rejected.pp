program FixedArrayAssignmentRejected;

type
  TLeft = array[0..1] of Integer;
  TRight = array[0..1] of Integer;

var
  Left: TLeft;
  Right: TRight;

begin
  Left := Right
end.
