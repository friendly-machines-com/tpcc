program PackedNestedWrite;

type
  TInner = packed record
    X: LongInt;
  end;
  TOuter = packed record
    Inner: TInner;
  end;

var
  R: TOuter;

begin
  R.Inner.X := 1;
end.
