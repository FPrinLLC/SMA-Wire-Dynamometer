%% Read the data
clear;
clc;
close all;

% This code is for reading data from multiple tests all at the same time.
% Plotting everything can take a minute so if you don't need to see the
% plots it would probably be best to just comment it out.

% Get the right test numbers (some did not work and therefore do not have
% data files, this is needed so the program does not get an error)
test_nums = [1, 2, 4:7, 9:15, 18:25, 27, 29:31, 33:36, 38:40, 44:59, 61:73, 76:95];
% Use the right force and voltage calibrations since it changed a few times
load_calibration = [linspace(1, 1, 7), linspace(2, 2, (40-7)), linspace(3, 3, max(test_nums)-40)];
voltage_calibration = [linspace(1, 1, 40), linspace(2, 2, (max(test_nums)-40))];
for j = test_nums
    % For each test:

    % File directory for test data
    filename = "S:\Shared Folders\a_Project\FP05_Prop_YY_NN_Internal Projects\FP05 - SMA Wire Engine\d_Test and Verification\Full Test\Full_Test_" + j + "_Edit.txt";
    % Open the file and read the data
    file = fopen(filename, "r");
    data = fscanf(file, "%i", [6, Inf]);

    % Use the calibration data to turn the raw values from the ESP32 into
    % meaninful data.
    time = data(1, :)/1000000;
    pos = data(2, :)*7.5/49;
    switch load_calibration(j)
        case 1
            load = data(3, :)*0.003167-0.952;
        case 2
            load = data(3, :)*0.003176-0.9567; % From calibration on 7/31
        case 3
            load = data(3, :)*0.003186-0.834; % For when VIN = ~27
    end
    current = data(4, :)*0.000847+0.017070;
    switch voltage_calibration(j)
        case 1
            voltage = data(5, :)*0.009067+0.197746;
        case 2
            voltage = data(5, :)*0.009110+0.175117; % For when VIN = ~27
    end
    on = data(6, :);
    power = current.*voltage;

    % Initialize variables for measuring the efficiency
    Ein = 0;
    Eout = 0;
    filteredEin = 0;
    filteredEout = 0;
    dE = 0;
    firstpull = false;

    for i = 1:length(time)-1
        % For each data point
        dE = power(i)*(time(i+1)-time(i)); % Calculate the energy put into the SMA for this data point
        % Using a left Riemann sum to calculate energy put in. 
        Ein = Ein + dE; % Add the energy put in to the total amount of energy put into the SMA
        if (on(i) == 1) 
            % I want to filter out when there was no power that was supposed to
            % be delivered to the SMA. The current readings were off at
            % essentially zero current so it would say there was a little power
            % consumption when the MOSFET was off when there was actually
            % essentially no power consumed at all.
            filteredEin = filteredEin + dE;
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
            end
        end
        if (firstpull == true) % Filtering for the "first pull" of the SMA.
            % Essentially just not counting the vibrations once the wire has
            % contracted and essentially done all of the work that can be used
            % in a meaningful way.
            if ((pos(i+1) - pos(i)) >= 0)
                % As long as the wire is still contracting/ as long as the
                % weight and everything is not moving down, add to the amount
                % of work done
                filteredEout = filteredEout + (pos(i+1)-pos(i))*load(i)/1000;
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
    % Calculate the efficiencies
    % "Raw efficiency", does not filter out when the circuit would say that
    % energy is put in, but there is actually no energy put into the wire. Also
    % includes the vibrations as work done
    eff(j) = Eout/Ein*100;
    % "Filtered energy in", filters out when the circuit would indicate that
    % energy is put in but there is actually no energy put into the wire. Also
    % includes the vibrations as work done
    filteredeff1(j) = Eout/filteredEin*100;
    % "Filtered energy in and out", same as the filtered energy in except does
    % not count the vibrations as work done.
    filteredeff2(j) = filteredEout/filteredEin*100;
    % Record the max load that the SMA went under for each test
    maxload(j) = max(load);
    % disp("I observed that there were measurements that looked like they were incorrect.");
    % disp("This was mostly in the current measurements, as there was a measured current of around 0.5A when in reality (I believe) there was no current flowing.");
    % disp("I have calculated efficiency with all measurements included(raw) and with only measurements from when the SMA wire was supposed to be receiving power(filtered Energy in).");
    % disp("I also noticed that there were vibrations after the SMA had fully contracted.");
    % disp("It would be very difficult to use the vibrations in a useful way, so I filtered these out (filtered Energy out)");
    fprintf("The raw efficiency is %.2f%%. \nThe filtered energy in efficiency is %.2f%%. \nThe filtered energy in and out efficiency is %.2f%%. \nThe max load is %.2fN.\n", eff(j), filteredeff1(j), filteredeff2(j), maxload(j));
    % fprintf("The raw efficiency is %f%%. The filtered energy in efficiency is %f%%. The filtered energy in and out efficiency is %f%%\n", eff, filteredeff1, filteredeff2);
    % Create plot and add labels
    figure(2*j-1);
    plot(time, power, time, load, time, pos);
    legendString = ["Power", "Force", "Displacement"];
    legend(legendString);
    xlabel("Time (s)");
    ylabel("Power (W), Displacement (mm), Force (N)");
    title("Data collected over time from test " + j);
    axis padded;
    % Use subplots, may be easier to read
    figure(2*j);
    subplot(3, 1, 1);
    plot(time, power);
    xlabel("Time (s)");
    ylabel("Power (W)");
    title("Data collected over time from test " + j);
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
    fclose(file);
end