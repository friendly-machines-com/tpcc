program InterfaceGuidNonstringRejected;

{$interfaces corba}

type
  IBad = interface
    [123]
  end;

begin
end.
