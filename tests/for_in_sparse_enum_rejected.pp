program ForInSparseEnumRejected;

type
  TSparse = (First = 1, Third = 3);

var
  Value: TSparse;

begin
  for Value in TSparse do
    Halt(1)
end.
