package info.cemu.Cemu;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;

import java.util.List;
import java.util.stream.Collectors;
import java.util.stream.Stream;

import info.cemu.Cemu.databinding.FragmentAudioSettingsBinding;

public class AudioSettingsFragment extends Fragment {

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        FragmentAudioSettingsBinding binding = FragmentAudioSettingsBinding.inflate(inflater, container, false);

        GenericRecyclerViewAdapter genericRecyclerViewAdapter = new GenericRecyclerViewAdapter();

        CheckboxRecyclerViewItem tvDeviceCheckbox = new CheckboxRecyclerViewItem(getString(R.string.tv),
                getString(R.string.tv_audio_description), NativeLibrary.getAudioDeviceEnabled(true),
                checked -> NativeLibrary.setAudioDeviceEnabled(checked, true));
        genericRecyclerViewAdapter.addRecyclerViewItem(tvDeviceCheckbox);

        var tvChannelsChoices = Stream.of(NativeLibrary.AUDIO_CHANNELS_MONO, NativeLibrary.AUDIO_CHANNELS_STEREO, NativeLibrary.AUDIO_CHANNELS_SURROUND)
                .map(channels -> new SelectionAdapter.ChoiceItem<>(t -> t.setText(NativeLibrary.channelsToResourceNameId(channels)), channels))
                .collect(Collectors.toList());
        int tvChannels = NativeLibrary.getAudioDeviceChannels(true);
        SelectionAdapter<Integer> tvChannelsSelectionAdapter = new SelectionAdapter<>(tvChannelsChoices, tvChannels);
        SingleSelectionRecyclerViewItem<Integer> tvChannelsModeSelection = new SingleSelectionRecyclerViewItem<>(getString(R.string.tv_channels),
                getString(NativeLibrary.channelsToResourceNameId(tvChannels)), tvChannelsSelectionAdapter,
                (channels, selectionRecyclerViewItem) -> {
                    NativeLibrary.setAudioDeviceChannels(channels, true);
                    selectionRecyclerViewItem.setDescription(getString(NativeLibrary.channelsToResourceNameId(channels)));
                });
        genericRecyclerViewAdapter.addRecyclerViewItem(tvChannelsModeSelection);

        SliderRecyclerViewItem tvVolumeSlider = new SliderRecyclerViewItem(getString(R.string.tv_volume),
                NativeLibrary.AUDIO_MIN_VOLUME,
                NativeLibrary.AUDIO_MAX_VOLUME,
                NativeLibrary.getAudioDeviceVolume(true),
                value -> NativeLibrary.setAudioDeviceVolume((int) value, true),
                value -> (int) value + "%");
        genericRecyclerViewAdapter.addRecyclerViewItem(tvVolumeSlider);

        CheckboxRecyclerViewItem padDeviceCheckbox = new CheckboxRecyclerViewItem(getString(R.string.gamepad),
                getString(R.string.gamepad_audio_description), NativeLibrary.getAudioDeviceEnabled(true),
                checked -> NativeLibrary.setAudioDeviceEnabled(checked, true));
        genericRecyclerViewAdapter.addRecyclerViewItem(padDeviceCheckbox);

        var gamepadChannelsChoices = List.of(new SelectionAdapter.ChoiceItem<>(t -> t.setText(NativeLibrary.channelsToResourceNameId(NativeLibrary.AUDIO_CHANNELS_STEREO)), NativeLibrary.AUDIO_CHANNELS_STEREO));
        int gamepadChannels = NativeLibrary.getAudioDeviceChannels(false);
        SelectionAdapter<Integer> gamepadChannelsSelectionAdapter = new SelectionAdapter<>(gamepadChannelsChoices, gamepadChannels);
        SingleSelectionRecyclerViewItem<Integer> gamepadChannelsModeSelection = new SingleSelectionRecyclerViewItem<>(getString(R.string.gamepad_channels),
                getString(NativeLibrary.channelsToResourceNameId(gamepadChannels)), gamepadChannelsSelectionAdapter,
                (channels, selectionRecyclerViewItem) -> {
                    NativeLibrary.setAudioDeviceChannels(channels, false);
                    selectionRecyclerViewItem.setDescription(getString(NativeLibrary.channelsToResourceNameId(channels)));
                });
        genericRecyclerViewAdapter.addRecyclerViewItem(gamepadChannelsModeSelection);

        SliderRecyclerViewItem padVolumeSlider = new SliderRecyclerViewItem(getString(R.string.pad_volume),
                NativeLibrary.AUDIO_MIN_VOLUME,
                NativeLibrary.AUDIO_MAX_VOLUME,
                NativeLibrary.getAudioDeviceVolume(false),
                value -> NativeLibrary.setAudioDeviceVolume((int) value, false),
                value -> (int) value + "%");
        genericRecyclerViewAdapter.addRecyclerViewItem(padVolumeSlider);

        binding.audioSettingsRecyclerView.setAdapter(genericRecyclerViewAdapter);

        return binding.getRoot();
    }
}