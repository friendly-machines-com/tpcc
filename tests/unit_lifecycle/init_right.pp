unit Init_Right;

interface

type
  TBase = class(TObject)
  public
    class destructor Finalize;
  end;

  TChild = class(TBase)
  public
    class destructor Finalize;
  end;

implementation

uses Init_Private;

class destructor TBase.Finalize;
begin
  Write('r')
end;

class destructor TChild.Finalize;
begin
  Write('s')
end;

initialization
  Write('C')
finalization
  Write('c')
end.
