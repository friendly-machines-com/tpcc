program class_var_visibility_boundary;

type
  TTempBaseNode = class
  protected
    class var TempInfoAccessor: Integer;
  protected
    procedure SetTempInfoFlags(const TempFlags: Integer);
  end;

begin
end.
