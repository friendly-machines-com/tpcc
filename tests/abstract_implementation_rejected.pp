program AbstractImplementationRejected;

type
  TBase = class(TObject)
  public
    procedure Missing; virtual; abstract;
  end;

procedure TBase.Missing;
begin
end;

begin
end.
