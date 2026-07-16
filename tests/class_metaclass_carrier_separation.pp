program ClassMetaclassCarrierSeparation;

type
  TPointerA = ^Integer;
  TPointerB = ^Integer;
  TExample = class
    procedure Select(Value: TPointerA); overload;
    class procedure Select(Value: TPointerB); overload;
  end;

begin
end.
