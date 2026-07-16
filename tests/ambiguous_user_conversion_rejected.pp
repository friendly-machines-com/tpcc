program AmbiguousUserConversionRejected;

{$interfaces corba}

type
  ILeft = interface
  end;
  IRight = interface
  end;
  TBoth = class(TObject, ILeft, IRight)
  end;
  TResult = record
    Value: Integer;
  end;

operator :=(Value: ILeft): TResult;
begin
  Result.Value := 1
end;

operator :=(Value: IRight): TResult;
begin
  Result.Value := 2
end;

var
  Source: TBoth;
  Destination: TResult;

begin
  Destination := Source
end.
