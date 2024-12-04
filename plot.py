import matplotlib.pyplot as plt
import pandas as pd
import sys

def plot_cwnd():
    # Read the CSV file
    df = pd.read_csv('CWND.csv')
    
    # Create the plot
    plt.figure(figsize=(10, 6))
    plt.plot(df['time'], df['CWND'], 'b-', label='CWND')
    
    # Add labels and title
    plt.xlabel('Time (seconds)')
    plt.ylabel('Congestion Window Size (packets)')
    plt.title('Congestion Window Size over Time')
    plt.grid(True)
    plt.legend()
    
    # Save the plot
    plt.savefig('cwnd_plot.png')
    plt.close()

if __name__ == "__main__":
    plot_cwnd() 