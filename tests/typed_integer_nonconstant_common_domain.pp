program TypedIntegerNonconstantCommonDomain;

var
  UnsignedValue: QWord;
  SignedValue: Int64;
  ResultValue: QWord;

begin
  UnsignedValue := 120;
  SignedValue := 12;
  ResultValue := UnsignedValue div SignedValue
end.
