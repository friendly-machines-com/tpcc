program AggregateTrueConstantAddressRejected;

type
  TConstants = record
    const Value = 7;
  end;

var
  Constants: TConstants;
  ValueAddress: ^Integer;

begin
  ValueAddress := @Constants.Value
end.
