program AccidentalVirtualCarrierOverrideRejected;

type
  TPointerA = ^Integer;
  TPointerB = ^Integer;

  TBase = class
    procedure Run(Value: TPointerA); virtual;
  end;

  TDerived = class(TBase)
    procedure Run(Value: TPointerB); virtual;
  end;

begin
end.
