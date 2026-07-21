import pandas as pd

def generate_clinical_report(week1_csv, week2_csv, subjective_claim_hours=16.8):
    # Load 7 days of 1-second telemetry data
    df_w1 = pd.read_csv(week1_csv)
    df_w2 = pd.read_csv(week2_csv)
    
    # Calculate days logged (Rows / 86400 seconds)
    w1_days = len(df_w1) / 86400
    w2_days = len(df_w2) / 86400
    
    # Extract True Wear-Time
    w1_compliant_hours = (df_w1['Status'] == 'Compliant').sum() / 3600
    w2_compliant_hours = (df_w2['Status'] == 'Compliant').sum() / 3600
    
    avg_w1 = w1_compliant_hours / w1_days
    avg_w2 = w2_compliant_hours / w2_days
    
    # Calculate Academic Metrics
    inflation_rate = ((subjective_claim_hours - avg_w1) / avg_w1) * 100
    improvement_rate = ((avg_w2 - avg_w1) / avg_w1) * 100
    
    print("--- AUTOMATED CLINICAL REPORT ---")
    print(f"Week 1 True Average Wear-Time: {avg_w1:.1f} hours/day")
    print(f"Compliance Gap (Inflation Rate): {inflation_rate:.1f}%")
    print("-" * 30)
    print(f"Week 2 Interactive Wear-Time: {avg_w2:.1f} hours/day")
    print(f"Adherence Improvement: {improvement_rate:.1f}%")

# Execute the analysis
generate_clinical_report("week1_silent_mode.csv", "week2_interactive_mode.csv")


