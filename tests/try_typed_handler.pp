program TryTypedHandler;

begin
  try
  except
    on E: TObject do
      E.Free
  end
end.
