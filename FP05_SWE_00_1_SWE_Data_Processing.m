%% Read the data
clear;
clc;
close all;

% This code is for reading data from just one test.
% For reading data from multiple tests, use
% FP05_SWE_00_2_SWE_Data_Processing.m

% File directory for the test data. 
filename = "S:\Shared Folders\a_Project\FP05_Prop_YY_NN_Internal Projects\FP05 - SMA Wire Engine\d_Test and Verification\Full Test\Full_Test_95_Edit.txt";
% Open the file and read the data
file = fopen(filename, "r");
data = fscanf(file, "%i", [6, Inf]);
% Save the data into a .mat file
save("SWE_Data", "data");
fclose(file);

%% Process the data

clear;
clc;
close all;

% This code is for processing the data from the .mat file created in the
% previous section. I have it in a separate section so that the data
% doesn't have to be reread every time I wanted to make sure things were
% working as I was writing the code.

load("SWE_Data.mat"); % Get the data

% Use calibration data to turn the raw data values from the ESP32 into
% meaningful data.
time = data(1, :)/1000000;
pos = data(2, :)*7.5/49;
% load = data(3, :)*0.003167-0.952;  % From initial calibration
% load = data(3, :)*0.003176-0.9567; % From calibration on 7/31
load = data(3, :)*0.003186-0.834; % For when VIN = ~27
current = data(4, :)*0.000847+0.017070;
% voltage = data(5, :)*0.009067+0.197746;   % From initial calibration
voltage = data(5, :)*0.009110+0.175117; % For when VIN = ~27
on = data(6, :);
power = current.*voltage;

% Initialize variables for measuring the efficiency
Ein = 0;
Eout = 0;
% filteredEin = 0;
% filteredEout = 0;
dE = 0;
weightedload = 0;
firstpull = false;

ctr = 1; % for looking at the data based on each time that the MOSFET is triggered/ each pulse.
% filteredEin = zeros(1, 100);
% filteredEout = zeros(1, 100);

for i = 1:length(time)-1
    % For each data point
    dE = power(i)*(time(i+1)-time(i)); % Calculate the energy put into the SMA for this data point
    % Using a left Riemann sum to calculate the energy put in.
    weightedload = weightedload + load(i)*(time(i+1)- time(i)); % For calculating the average force on the SMA
    Ein = Ein + dE; % Add the energy put in to the total amount of energy put into the SMA
    if (on(i) == 1) 
        % I want to filter out when there was no power that was supposed to
        % be delivered to the SMA. The current readings were off at
        % essentially zero current so it would say there was a little power
        % consumption when the MOSFET was off when there was actually 
        % essentially no power consumed at all.
        if (firstpull == false)
            % I want to know when is the "first pull" when the wire is
            % triggered. This is because of vibrations that happen after
            % the wire is fully contracted. My initial way of measuring
            % work done included these vibrations as work done. However,
            % the only work that can really be harnessed and used in a
            % meaningful way is the work done from the first contraction of
            % the wire. Another way to look at it is that if we are using 
            % all of the energy from the initial contraction, then there 
            % would be no energy to create vibrations. 
            firstpull = true;
            
            if (ctr > 1)
                % Output the efficiency of the last contraction into the 
                % command window and increase the counter
                disp(filteredEout(ctr)/filteredEin(ctr)*100);
                ctr = ctr + 1;
            end
            % Set the amount of energy for the contraction equal to zero to
            % start
            filteredEin(ctr) = 0;
            filteredEout(ctr) = 0;
        end
        % Add to the "filtered energy consumed" if the MOSFET is supposed
        % to be on.
        filteredEin(ctr) = filteredEin(ctr) + dE;        
    end
    if (firstpull == true) % Filtering for the "first pull" of the SMA.
        % Essentially just not counting the vibrations once the wire has
        % contracted and essentially done all of the work that can be used
        % in a meaningful way. 
        if ((pos(i+1) - pos(i)) >= 0)
            % As long as the wire is still contracting/ as long as the
            % weight and everything is not moving down, add to the amount 
            % of work done
            filteredEout(ctr) = filteredEout(ctr) + (pos(i+1)-pos(i))*load(i)/1000;
            % Using a left riemann sum to calculate the work done
        else
            % If the wire is relaxing/ things are moving down, then
            % consider the "first pull" finished.
            firstpull = false;
        end
    end
    if (pos(i+1) > pos(i)) % Filtering for when the load is moving up.
        % When its moving down is work done by gravity, not the SMA. I
        % don't want this to be counted as negative work done by the SMA.
        Eout = Eout + (pos(i+1)-pos(i))*load(i)/1000;
        % This does not filter out the vibrations.
    end
end
disp(weightedload/(time(i)-time(1))); % Show the average force on the wire
% Calculate the efficiencies

% "Raw efficiency", does not filter out when the circuit would say that
% energy is put in, but there is actually no energy put into the wire. Also
% includes the vibrations as work done
eff = Eout/Ein*100; 

% "Filtered energy in", filters out when the circuit would indicate that
% energy is put in but there is actually no energy put into the wire. Also
% includes the vibrations as work done
filteredeff1 = Eout/sum(filteredEin)*100;

% "Filtered energy in and out", same as the filtered energy in except does
% not count the vibrations as work done.
filteredeff2 = sum(filteredEout)/sum(filteredEin)*100;

% disp("I observed that there were measurements that looked like they were incorrect.");    
% disp("This was mostly in the current measurements, as there was a measured current of around 0.5A when in reality (I believe) there was no current flowing.");
% disp("I have calculated efficiency with all measurements included(raw) and with only measurements from when the SMA wire was supposed to be receiving power(filtered Energy in).");
% disp("I also noticed that there were vibrations after the SMA had fully contracted.");
% disp("It would be very difficult to use the vibrations in a useful way, so I filtered these out (filtered Energy out)");
fprintf("The raw efficiency is %.2f%%. \nThe filtered energy in efficiency is %.2f%%. \nThe filtered energy in and out efficiency is %.2f%%. \nThe max load is %.2fN.\n", eff, filteredeff1, filteredeff2, max(load));
% fprintf("The raw efficiency is %f%%. The filtered energy in efficiency is %f%%. The filtered energy in and out efficiency is %f%%\n", eff, filteredeff1, filteredeff2);

% Create plot and add labels
plot(time, power, time, load, time, pos);
legendString = ["Power", "Force", "Displacement"];
legend(legendString);
xlabel("Time (s)");
ylabel("Power (W), Displacement (mm), Force (N)");
title("Data collected over time");
axis padded;
% Use subplots, may be easier to read
figure(2);
subplot(3, 1, 1);
plot(time, power);
xlabel("Time (s)");
ylabel("Power (W)");
title("Data collected over time");
axis padded;
subplot(3, 1, 2);
plot(time, pos);
xlabel("Time (s)");
ylabel("Displacement (mm)");
axis padded;
subplot(3, 1, 3);
plot(time, load);
xlabel("Time (s)");
ylabel("Force (N)");
axis padded;