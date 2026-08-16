program TypedStaticAddressArithmeticRejected;

var
  GlobalValue: LongInt;

const
  AdvancedAddress: ^LongInt = @GlobalValue + 1;

begin
end.
