program function_pointer_method_to_global_rejected;

type
  TPlain = procedure(Value: Integer);
  TBox = object
    procedure Run(Value: Integer);
  end;

var
  Plain: TPlain;
  Box: TBox;

procedure TBox.Run(Value: Integer);
begin
end;

begin
  Plain := @Box.Run
end.
