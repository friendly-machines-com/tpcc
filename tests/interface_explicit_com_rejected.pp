program InterfaceExplicitComRejected;

{$interfaces corba}

type
  ISupported = interface
  end;

{$interfaces com}

type
  IUnsupported = interface
  end;

begin
end.
