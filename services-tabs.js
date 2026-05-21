const serviceTabs = document.querySelectorAll('.services-tab');
const servicePanels = document.querySelectorAll('.services-panel');

if (serviceTabs.length && servicePanels.length) {
    serviceTabs.forEach((tab) => {
        tab.addEventListener('click', () => {
            const target = tab.dataset.serviceTab;

            serviceTabs.forEach((t) => {
                t.classList.remove('is-active');
                t.setAttribute('aria-selected', 'false');
            });

            servicePanels.forEach((panel) => {
                panel.classList.toggle('is-active', panel.dataset.servicePanel === target);
            });

            tab.classList.add('is-active');
            tab.setAttribute('aria-selected', 'true');
        });
    });
}
