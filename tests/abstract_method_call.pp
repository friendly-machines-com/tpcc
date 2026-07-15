program AbstractMethodCall;

type
  TBase = class(TObject)
  public
    procedure Missing; virtual; abstract;
  end;

var
  Value: TBase;

begin
  Value := TBase.Create;
  Value.Missing
end.
