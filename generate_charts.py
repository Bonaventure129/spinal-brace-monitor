import pandas as pd
import matplotlib.pyplot as plt

# 1. Load the generated CSVs
df_w1 = pd.read_csv('week1_silent_mode.csv')
df_w2 = pd.read_csv('week2_interactive_mode.csv')

# Helper function to ensure the order is always [Compliant, Removed]
def get_ordered_counts(counts):
    return [counts.get('Compliant', 0), counts.get('Removed', 0)]

# Style settings
labels = ['Compliant (Worn)', 'Removed (Off)']
colors = ['#00aa66', '#cc0033'] # Neon Green & Neon Red

# --- Generate Week 1 Chart ---
# Increased width from 7 to 9 for more text room
fig1, ax1 = plt.subplots(figsize=(9, 6)) 
fig1.patch.set_facecolor('#080c14')

ax1.pie(get_ordered_counts(df_w1['Status'].value_counts()), labels=labels, autopct='%1.1f%%', startangle=90, colors=colors, 
        textprops={'color':"white", 'weight':'bold', 'fontsize': 13}, wedgeprops={'edgecolor': 'black'})
ax1.set_title('Week 1: Silent Mode\n(Avg 11.8 hrs/day)', color='white', weight='bold', fontsize=16)

# Added bbox_inches='tight' to prevent text cutoff
plt.savefig('week1_pie_chart.png', facecolor=fig1.get_facecolor(), edgecolor='none', dpi=300, bbox_inches='tight')
plt.close(fig1)

# --- Generate Week 2 Chart ---
# Increased width from 7 to 9 for more text room
fig2, ax2 = plt.subplots(figsize=(9, 6))
fig2.patch.set_facecolor('#080c14')

ax2.pie(get_ordered_counts(df_w2['Status'].value_counts()), labels=labels, autopct='%1.1f%%', startangle=90, colors=colors, 
        textprops={'color':"white", 'weight':'bold', 'fontsize': 13}, wedgeprops={'edgecolor': 'black'})
ax2.set_title('Week 2: Interactive Mode\n(Avg 19.2 hrs/day)', color='white', weight='bold', fontsize=16)

# Added bbox_inches='tight' to prevent text cutoff
plt.savefig('week2_pie_chart.png', facecolor=fig2.get_facecolor(), edgecolor='none', dpi=300, bbox_inches='tight')
plt.close(fig2)

print("Fixed charts successfully saved as 'week1_pie_chart.png' and 'week2_pie_chart.png'!")
