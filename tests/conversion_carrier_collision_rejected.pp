program ConversionCarrierCollisionRejected;

type
  TSource = record
    Value: Integer;
  end;
  TResultA = record
    Value: Integer;
  end;
  TResultB = record
    Value: Integer;
  end;

operator :=(const Value: TSource): TResultA; forward;
operator :=(const Value: TSource): TResultB; forward;

begin
end.
