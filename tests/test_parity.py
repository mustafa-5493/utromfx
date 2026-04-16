def test_g1_policy_parity():
    model = MyG1Policy() # Your trained actor
    codegen = AxonCodegen(model)
    header = codegen.generate_header()
    
    validator = AxonValidator()
    # If this fails, the GitHub Action will turn Red ❌
    assert validator.verify(model, header, input_dim=45)