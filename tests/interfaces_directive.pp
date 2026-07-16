program InterfacesDirective;

{$interfaces com}
{$interfaces corba}

type
  IFirst = interface
    procedure First;
  end;

{$interfaces default}
{$interfaces corba}

type
  ISecond = interface(IFirst)
    procedure Second;
  end;

begin
end.
