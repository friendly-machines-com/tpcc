program InterfaceGuid;

type
  IBase = interface
    ['{51BE3F89-C9C5-4965-9C83-AE7490C92E3E}']
    procedure Base;
  end;

  IChild = interface(IBase)
    ['{C056F0DD-62B1-4612-86C7-2D39944C4437}']
    procedure Child;
  end;

  IEmptyGuid = interface
    ['']
  end;

begin
end.
