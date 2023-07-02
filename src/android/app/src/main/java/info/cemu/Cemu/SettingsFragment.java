package info.cemu.Cemu;

import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.SpinnerAdapter;

import java.util.ArrayList;
import java.util.List;

import info.cemu.Cemu.databinding.FragmentSettingsBinding;

import info.cemu.Cemu.databinding.SettingsCheckboxBinding;
import info.cemu.Cemu.databinding.SettingsHeaderBinding;
import info.cemu.Cemu.databinding.SettingsCheckboxBinding;
import info.cemu.Cemu.databinding.SettingsSpinnerBinding;

public class SettingsFragment extends Fragment {
    protected FragmentSettingsBinding binding;

    public SettingsFragment() {
    }

//    public abstract int getSettingsNameId();

    protected interface OnCheckBoxStateListener {
        void onStateChanged(boolean checked);
    }

    protected interface OnItemSelectedPositionListener {
        void onItemSelected(int position);
    }

    protected void addCheckBox(String title, String description, boolean initialValue, OnCheckBoxStateListener checkBoxStateListener) {
        View checkboxLayout = getLayoutInflater().inflate(R.layout.settings_spinner, null);
        binding.settingsLayout.addView(checkboxLayout);
        SettingsCheckboxBinding checkboxBinding = SettingsCheckboxBinding.bind(checkboxLayout);
        checkboxBinding.textviewCheckboxName.setText(title);
        checkboxBinding.checkbox.setChecked(initialValue);
        checkboxBinding.textviewCheckboxDescription.setText(description);
        checkboxLayout.setOnClickListener(v -> {
            boolean isChecked = !checkboxBinding.checkbox.isChecked();
            checkboxBinding.checkbox.setChecked(isChecked);
            checkBoxStateListener.onStateChanged(isChecked);
        });
    }

//    protected void addSeparator() {
//        View divider = getLayoutInflater().inflate(R.layout.divider_layout, null);
//        binding.settingsLayout.addView(divider);
//    }

    protected void addHeader(String title) {
        View headerLayout = getLayoutInflater().inflate(R.layout.settings_header, null);
        binding.settingsLayout.addView(headerLayout);
        SettingsHeaderBinding headerBinding = SettingsHeaderBinding.bind(headerLayout);
        headerBinding.settingsTextviewHeaderName.setText(title);
    }

    protected void addSpinner(String title, List<String> items, int initialPosition, OnItemSelectedPositionListener itemSelectedPositionListener) {
        View spinnerLayout = getLayoutInflater().inflate(R.layout.settings_spinner, null);
        binding.settingsLayout.addView(spinnerLayout);
        SettingsSpinnerBinding spinnerBinding = SettingsSpinnerBinding.bind(spinnerLayout);
        spinnerBinding.textviewSpinnerName.setText(title);
        spinnerLayout.setOnClickListener(v -> spinnerBinding.spinner.performClick());
        ArrayAdapter<String> adapter = new ArrayAdapter<>(getContext(), android.R.layout.simple_spinner_item, items);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinnerBinding.spinner.setAdapter(adapter);
        spinnerBinding.spinner.setSelection(initialPosition);
        spinnerBinding.spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                itemSelectedPositionListener.onItemSelected(position);
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });
    }

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        binding = FragmentSettingsBinding.inflate(inflater, container, false);
        return binding.getRoot();
    }
}