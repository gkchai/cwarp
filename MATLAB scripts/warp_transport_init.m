%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% MATLAB scrip to be run before running the transprort code 
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


clear;
USE_AGC = false;

NUMNODES = 16;

%Create a vector of node objects
nodes = wl_initNodes(NUMNODES, [0: NUMNODES-1]);


%Create a UDP broadcast trigger and tell each node to be ready for it
eth_trig = wl_trigger_eth_udp_broadcast;
wl_triggerManagerCmd(nodes,'add_ethernet_trigger',[eth_trig]);


[RFA,RFB, RFC, RFD] = wl_getInterfaceIDs(nodes(1));

%Set up the interface for the experiment
wl_interfaceCmd(nodes,RFA+RFB,'tx_gains',2,30);
wl_interfaceCmd(nodes,RFA+RFB,'channel',2.4,11);

wl_interfaceCmd(nodes,RFA+RFB,'rx_gain_mode','manual');
RxGainRF = 1; %Rx RF Gain in [1:3]
RxGainBB = 15; %Rx Baseband Gain in [0:31]
wl_interfaceCmd(nodes,RFA+RFB,'rx_gains',RxGainRF,RxGainBB);


txLength = nodes(1).baseband.txIQLen;
rxLength = nodes(1).baseband.rxIQLen;

%Set up the baseband for the experiment
wl_basebandCmd(nodes,'tx_delay',0);
wl_basebandCmd(nodes,'tx_length',32767); % txLength needs to be set for rx nodes as well, is it a bug ??
wl_basebandCmd(nodes,'rx_length',32767);

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Depending on read or write, send the interface commands accordingly 
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% node_tx = nodes(1:NUMNODES);
node_rx = nodes(1:NUMNODES);

%node_rx = nodes(NUMNODES/2 + 1:end);


% wl_interfaceCmd(node_tx,RFA+RFB,'tx_en');
wl_interfaceCmd(node_rx,RFA+RFB,'rx_en');

% wl_basebandCmd(node_tx,RFA+RFB,'tx_buff_en');
wl_basebandCmd(node_rx,RFA+RFB,'rx_buff_en');

% eth_trig.send();


